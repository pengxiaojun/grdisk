#include "extractor.h"
#include <QDebug>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusReply>

extractor::extractor()
{
    //add os related mount path
    sysmountpoints.append("/");
    sysmountpoints.append("/home");
    sysmountpoints.append("/tmp");
    sysmountpoints.append("/opt");
    sysmountpoints.append("/srv");
    sysmountpoints.append("/var");
    sysmountpoints.append("/boot");
}

extractor::~extractor()
{

}

void extractor::extract(managedobject *mo, ManagedObjectList& objlist)
{
    QList<QDBusObjectPath> objpathlist = objlist.keys();

    foreach(QDBusObjectPath objpath, objpathlist)
    {
        InterfaceList il = objlist.value(objpath);

        QString path = objpath.path();
        //qDebug() << path;
        if (path.startsWith(UDISK2_BLOCK_PATH))
        {
            blockdevice *dev = extract_block(path, il);
            mo->blockdev[path] = dev;
        }
        else if (path.startsWith(UDISK2_DRIVE_PATH))
        {
            drivedevice *dev = extract_drive(path, il);
            mo->drivedev[path] = dev;
        }
        else{
            //do nothing
        }
    }
    mark_device_flag(mo);
}

blockdevice* extractor::extract_block(QString path, const InterfaceList &il)
{
    blockdevice *blockdev = new blockdevice();
    blockdev->path = path;

    QList<QString> interfaces = il.keys();
    foreach(QString interface, interfaces)
    {
        //qDebug() << "   block interface" << interface;
        blockdev->interface.append(interface);
        QVariantMap vm = il.value(interface);

        if (interface == UDISK2_BLOCK_INTERFACE)
        {
            //block property
            foreach(QString property, vm.keys())
            {
                QVariant v = vm.value(property);
                //qDebug() << "        property" << property;

                if (property == "Configuration")
                {
                    /*InterfaceList il = qdbus_cast<InterfaceList>(v);
                    qDebug() << "           got conf" << il.size();
                    foreach(QString key, il.keys())
                    {
                        //key is fstab
                        QVariantMap vm = il.value(key);
                        qDebug() << "       config" << key << vm.keys().size();

                        foreach(QString k, vm.keys())
                        {

                            QVariant v = vm.value(k);

                            QString str(v.toByteArray());
                            qDebug() << key << v.type() << str;
                        }
                    }*/
                }
                else if (property == "IdUUID")
                {
                    blockdev->uuid = v.toString();
                }
                else if (property == "IdUsage")
                {
                }
                else if (property == "Drive")
                {
                    QDBusObjectPath drivePath = qdbus_cast<QDBusObjectPath>(v);
                    blockdev->drivePath = drivePath.path();
                }
                else if (property == "ReadOnly")
                {
                    blockdev->readonly = v.toBool();
                }
                else if (property == "Size")
                {
                    blockdev->size = v.toULongLong();
                }
                else if (property == "Id")
                {
                    QString id = v.toString();
                    if (id.startsWith(DEVICE_ID_PREFIX))
                        id = id.remove(DEVICE_ID_PREFIX);
                    if (id.startsWith(DEVICE_UUID_PREFIX))
                        id = id.remove(DEVICE_UUID_PREFIX);
                    blockdev->id = id;
                }
                else if (property == "Symlinks")
                {
                    QList<QByteArray> bytelist = qdbus_cast<QList<QByteArray> >(v);

                    for (int i = 0; i<bytelist.size(); ++i)
                    {
                        QByteArray bytes = bytelist.at(i);
                        QString symlink(bytes);
                        //qDebug() << i << "<<" << bytelist.size() <<  "block device" << QString(bytes);
                        blockdev->symlinks.append(symlink);
                    }
                }
                else if (property == "PreferredDevice")
                {
                    blockdev->preferredDev = QString(v.toByteArray());
                    //qDebug() << ">> PreferredDevice" << blockdev->preferredDev;
                }
                else if (property == "Device")
                {
                    blockdev->device = QString(v.toByteArray());
                    blockdev->isram = blockdev->device.startsWith(DEVICE_RAM_PREFIX);
                    blockdev->isloop = blockdev->device.startsWith(DEVICE_LOOP_PREFIX);
                }
            }            
            //qDebug() << "------------device" <<blockdev->device <<blockdev->id << "part num" << blockdev->partnum;
        }
        else if (interface == UDISK2_FS_INTERFACE)
        {
            blockdev->isdrive = false;
            //block property
            foreach(QString property, vm.keys())
            {
                QVariant v = vm.value(property);
                if (property == "MountPoints")
                {
                    QList<QByteArray> bytelist = qdbus_cast<QList<QByteArray> >(v);

                    for(int i = 0; i<bytelist.size(); ++i)
                    {
                        QString mountpoint = QString(bytelist.at(i));
                        blockdev->mountpoints.append(mountpoint);
                        //qDebug() << i << ">> mount point" << mountpoint;

                        if (sysmountpoints.contains(mountpoint))
                        {
                            blockdev->isboot = true;
                        }
                    }
                }
            }
        }
        else if (interface == UDISK2_PART_INTERFACE)
        {
            blockdev->isdrive = false;
            //block property
            foreach(QString property, vm.keys())
            {
                QVariant v = vm.value(property);
                if (property == "Number")
                {
                    blockdev->partnum = v.toUInt();
                }
                else if (property == "Size")
                {
                    blockdev->partsize = v.toULongLong();
                }
                else if (property == "Offset")
                {
                    blockdev->partoffset = v.toULongLong();
                }
                else if (property == "Name")
                {
                    blockdev->partname = v.toString();
                }
                else if (property == "Type")
                {
                    blockdev->parttype = v.toString();
                }
                else if (property == "UUID")
                {
                    blockdev->partuuid = v.toString();
                }
                else if (property == "Table")
                {
                    QDBusObjectPath partpath = qdbus_cast<QDBusObjectPath>(v);
                    blockdev->parttable = partpath.path();
                    //qDebug() << ">> part table path" << blockdev->parttable;
                }
            }
        }
        else if (interface == UDISK2_PARTAB_INTERFACE)
        {
            blockdev->isdrive = true;
            //partition table property
            foreach(QString property, vm.keys())
            {
                QVariant v = vm.value(property);
                if (property == "Type")
                {
                    blockdev->parttabtype = v.toString();
                    //qDebug() << "part table type " << blockdev->parttabtype;
                }
            }
        }
        else
        {

        }
    }//end foreach
    return blockdev;
}

drivedevice* extractor::extract_drive(QString path, const InterfaceList &il)
{
    drivedevice *drivedev = new drivedevice();
    drivedev->path = path;

    QList<QString> interfaces = il.keys();
    foreach(QString interface, interfaces)
    {
        //qDebug() << "   drive interface" << interface;
        QVariantMap vm = il.value(interface);

        if (interface == UDISK2_DRIVE_INTERFACE)
        {
            foreach(QString property, vm.keys())
            {
                //qDebug() << "       property" << property;
                QVariant v = vm.value(property);
                if (property == "Vendor")
                {
                    drivedev->vendor =v.toString();
                }
                else if (property == "Model")
                {
                    drivedev->model = v.toString();
                }
                else if (property == "Serial")
                {
                    drivedev->serial = v.toString();
                }
                else if (property == "Revision")
                {
                    drivedev->revision = v.toString();
                }
                else if (property == "Configuration")
                {

                }
                else if (property == "Id")
                {
                    drivedev->id = v.toString();
                }
                else if (property == "ConnectionBus")
                {
                    drivedev->connectbus = v.toString();
                }
                else if (property == "Removable")
                {
                    drivedev->removable = v.toBool();
                }
                else if(property == "Ejectable")
                {
                    drivedev->ejectable = v.toBool();
                }
                else if (property == "CanPowerOff")
                {
                    drivedev->canpoweroff = v.toBool();
                }
            }
            //qDebug() << "drive" <<drivedev->model <<drivedev->connectbus <<drivedev->path <<drivedev->vendor;
        }
        else if (interface == UDISK2_ATA_INTERFACE)
        {
            //unused
        }
    }
    return drivedev;
}

void extractor::mark_device_flag(managedobject *mo)
{
    QList<blockdevice*> blocks = mo->blockdev.values();
    QList<drivedevice*> drives = mo->drivedev.values();
    QString bootPartition;
    QString bootDevice;
    //find boot partition
    foreach(blockdevice* block, blocks)
    {
        if (block->isboot)
        {
            bootPartition = block->id;
        }
        //mark usb and drive id
        foreach(drivedevice *drive, drives)
        {
            if (drive->path == block->drivePath)
            {
                if (block->id.isEmpty())    //id of usb drive may be empty
                {
                    block->id = drive->id;
                }
                block->driveId = drive->id;
                block->isusb = (drive->connectbus.toLower() == USB_CONNECTTED_BUS);
                break;
            }
        }
        //mark block drive device name for partition devic
        if (!block->isdrive)
        {
            foreach(blockdevice *block2, blocks)
            {
                if (block2->drivePath == block->drivePath && block2->isdrive)
                {
                    block->drive = block2->device;
                    break;
                }
            }
        }
    }
    if (bootPartition.isEmpty())
    {
        return;
    }
    //find boot device
    foreach(blockdevice* block, blocks)
    {
        if (block->isdrive && bootPartition.indexOf(block->id) != -1)
        {
            bootDevice = block->id;
            break;
        }
    }
    if (bootDevice.isEmpty())
    {
        return;
    }
    //find and set all bool partition flag
    foreach(blockdevice* block, blocks)
    {
        if (block->id.startsWith(bootDevice))
        {
            block->isboot = true;
        }
    }
}
