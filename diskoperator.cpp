#include "diskoperator.h"
#include <QDebug>
#include <QProcess>
#include "util.h"

diskoperator::diskoperator()
{
}

diskoperator::~diskoperator()
{

}

int diskoperator::list(const managedobject &mo)
{
    //execute dport to get disk physical pci interface
    QMap<QString,int> ports;
    QMap<QString, dfentry> dfentries;
    cmdtool.df(dfentries);
    cmdtool.dport(ports);

    QList<blockdevice*> blocks = mo.blockdev.values();

    QStringList output;
    QString item;
    foreach(blockdevice *block, blocks)
    {
        if (!g_option.show_boot_device && block->isboot)
            continue;
        if (!g_option.show_loop_device && block->isloop)
            continue;
        if (!g_option.show_ram_device && block->isram)
            continue;

        int port = getport(block, ports);

        if (block->isdrive)
        {
            item = QString("%1 disk %2 %3 %4 %5 %6")
                    .arg(port)              //port number
                    .arg(block->device)     //device name
                    .arg(block->id)         //block device id
                    .arg(block->device)     //drive name
                    .arg(block->driveId)    //drive id
                    .arg(block->size);      //block device size
        }
        else
        {
            /*QString drive;
            QString partnum = QString::number(block->partnum);
            if (block->device.endsWith(partnum))
            {
                int length = partnum.length();
                drive = block->device.left(block->device.length() - length);
            }*/
            const dfentry& entry = dfentries.value(block->device);
            item = QString("%1 part %2 %3 %4 %5 %6 %7 %8")
                    .arg(port)
                    .arg(block->device)            //device name
                    .arg(block->id)
                    .arg(block->drive)             //drive name
                    .arg(block->driveId)
                    .arg(block->partsize)
                    .arg(block->mountpoints.size() > 0 ? block->mountpoints[0] : "NONE")
                    .arg(entry.used);
        }
        output.append(item);
    }

    foreach(QString item, output)
    {
        qDebug() << qPrintable(item);
    }
    return 0;
}

int diskoperator::partcreate(const managedobject &mo, QString disk, qulonglong size)
{
    Q_UNUSED(size);
    const blockdevice *block = lookup_block(mo, disk);
    if (block == NULL)
    {
        return -1;
    }
    return cmdtool.create_part(block);
}

int diskoperator::partdelete(const managedobject &mo, QString part)
{
    const blockdevice *block = lookup_block(mo, part);
    if (block == NULL)
    {
        return -1;
    }
    return cmdtool.remove_part(block);
}

int diskoperator::format(const managedobject &mo, QString part)
{
    const blockdevice *block = lookup_block(mo, part);
    if (block == NULL)
    {
        return -1;
    }
    return cmdtool.format_part(block);
}

int diskoperator::mount(const managedobject &mo, QString part, QString mountpoint, QString uid, QString gid)
{
    const blockdevice *block = lookup_block(mo, part);
    if (block == NULL)
    {
        return -1;
    }
    return cmdtool.mount_part(block, mountpoint, uid, gid);
}

int diskoperator::unmount(const managedobject &mo, QString part)
{
    const blockdevice *block = lookup_block(mo, part);
    if (block == NULL)
    {
        return -1;
    }
    return cmdtool.umount_part(block);
}

int diskoperator::automount(const managedobject &mo, QString part, QString mountpoint)
{
    const blockdevice *block = lookup_block(mo, part);
    if (block == NULL)
    {
        return -1;
    }
    return cmdtool.automnt_part(block, mountpoint);
}

int diskoperator::deautomount(const managedobject &mo, QString part)
{
    const blockdevice *block = lookup_block(mo, part);
    if (block == NULL)
    {
        return -1;
    }
    return cmdtool.deautomnt_part(block);
}

int diskoperator::getport(blockdevice *block, QMap<QString,int> ports)
{
    foreach(QString dev, ports.keys())
    {
        if (block->device.startsWith(dev))
        {
            return ports.value(dev);
        }
    }
    return -1;
}

const blockdevice *diskoperator::lookup_block(const managedobject &mo, QString device)
{
    foreach(const blockdevice *block, mo.blockdev.values())
    {
        if (block->device == device)
        {
            return block;
        }
    }
    return NULL;
}
