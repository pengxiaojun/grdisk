#ifndef DISKOPERATOR_H
#define DISKOPERATOR_H

#include "devicedef.h"
#include "commandtool.h"

class diskoperator
{
public:
    diskoperator();
    ~diskoperator();

    int list(const managedobject& mo);
    int partcreate(const managedobject& mo, QString disk, qulonglong size=0);
    int partdelete(const managedobject& mo, QString part);
    int format(const managedobject& mo, QString part);
    int mount(const managedobject& mo, QString part, QString mountpoint,QString uid, QString gid="");
    int unmount(const managedobject& mo, QString part);
    int automount(const managedobject& mo, QString part, QString mountpoint);
    int deautomount(const managedobject& mo, QString part);
private:
    int getport(blockdevice *block, QMap<QString,int> ports);
    const blockdevice* lookup_block(const managedobject &mo, QString device);

private:
    commandtool cmdtool;
};

#endif // DISKOPERATOR_H
