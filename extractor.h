#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include "devicedef.h"

class extractor
{
public:
    extractor();
    ~extractor();

    void extract(managedobject *obj, ManagedObjectList& objlist);
private:

    blockdevice* extract_block(QString path, const InterfaceList& il);
    drivedevice* extract_drive(QString path, const InterfaceList& il);
    void mark_device_flag(managedobject* mo);
private:
    QStringList sysmountpoints;
};

#endif // EXTRACTOR_H
