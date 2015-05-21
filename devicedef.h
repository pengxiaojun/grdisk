#ifndef DEVICEDEF_H
#define DEVICEDEF_H

#include <QMap>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDBusObjectPath>
#include <QDBusMetaType>

//error code
#define GE_OK           0
#define GE_ERROR        -1
#define NO_FIND_DEVICE  -2
#define ERR_SIZE        -3
#define ERR_MOUNTED     -4
#define ERR_PART_SCHEMA -5
#define ERR_NO_UDISK2   -6


#define DBUS_INTERFACE          "org.freedesktop.DBus"
#define UDISK2_INTERFACE        "org.freedesktop.UDisks2"
#define UDSIS2_OBJ_INTERFACE    "org.freedesktop.DBus.ObjectManager"
#define UDISK2_PATH             "/org/freedesktop/UDisks2"
#define UDISK2_MANAGER_PATH     "/org/freedesktop/UDisks2/Manager"
#define UDISK2_BLOCK_PATH       "/org/freedesktop/UDisks2/block_devices"
#define UDISK2_DRIVE_PATH       "/org/freedesktop/UDisks2/drives"
#define UDISK2_MDRAID_PATH      "/org/freedesktop/UDisks2/mdraid"

//block device interface
#define UDISK2_BLOCK_INTERFACE  "org.freedesktop.UDisks2.Block"
#define UDISK2_FS_INTERFACE     "org.freedesktop.UDisks2.Filesystem"
#define UDISK2_PART_INTERFACE   "org.freedesktop.UDisks2.Partition"
#define UDISK2_PARTAB_INTERFACE "org.freedesktop.UDisks2.PartitionTable"

//drive device interface
#define UDISK2_DRIVE_INTERFACE  "org.freedesktop.UDisks2.Drive"
#define UDISK2_ATA_INTERFACE    "org.freedesktop.UDisks2.Drive.Ata"

//mdraid device interval
#define UDISK2_MDRAID_INTERFACE "org.freedesktop.UDisks2.MDRaid"

#define DEVICE_ID_PREFIX        "by-id-"
#define DEVICE_UUID_PREFIX      "by-uuid-"
#define DEVICE_PATH_PREFIX      "/dev/disk/by-id/"

#define DEVICE_RAM_PREFIX       "/dev/ram"
#define DEVICE_LOOP_PREFIX      "/dev/loop"
#define USB_CONNECTTED_BUS      "usb"

/* a{sv}: is a dict of {string:variant} pairs
 *        QVariantMap fit this signature
 *
 * a{sa{sv}} is a dict of string:a{sv} pairs
 *        QMap<QString, QVariantMap> fit this signature
 *
 * a{oa{sa{sv}}} is a dict of objectpath:a{sa{sv}} pairs
 *       QMap<QDBusObjectPath, QMap<QString, QVariantMap> >
 *
 */

typedef QMap<QString, QVariantMap> InterfaceList;
typedef QMap<QDBusObjectPath, InterfaceList> ManagedObjectList;

struct MDRaidMember
{
    QDBusObjectPath block;
    qint32 slot;
    QStringList state;
    QVariantMap expansion;
};

Q_DECLARE_METATYPE(MDRaidMember)
Q_DECLARE_METATYPE(QList<MDRaidMember>)

struct grdisk_opt_t
{
    bool show_usb_device;
    bool show_boot_device;
    bool show_ram_device;
    bool show_loop_device;
    bool use_udisk;
    bool verbose;
    bool debug;
};

struct dfentry
{
    QString dev;
    qulonglong used;
    qulonglong avail;
    QString mountpoint;
};

class mdraiddevice
{
public:
    mdraiddevice();
    ~mdraiddevice();
public:
    QString path;
    QString uuid;
    QString name;
    QString level;
    int numDevices;
    int degraded;
    qulonglong size;
    QString syncAction;
    double syncCompleted;
    qulonglong syncRate;
    qulonglong syncRemainingTime;
    qulonglong chunkSize;
    QString bitmapLocation;
    QList<MDRaidMember> members;

};

class drivedevice
{
public:
    drivedevice();
    ~drivedevice();

public:
    QString id;     //drive id. equals driveid in blockdevice class
    QString path;   //drivate path. equals drivePath in blockdevice class
    QStringList interface;
    QString vendor;
    QString model;
    QString serial;
    QString revision;
    QString connectbus;
    bool removable;
    bool ejectable;
    bool canpoweroff;
};

class blockdevice
{
public:
    blockdevice();
    ~blockdevice();
public:
    //block property
    QString path;
    QString id;
    QString device;         //such /dev/sdx1 /dev/sdx. such as:{/dev/sda, /dev/sdb.../dev/sdz, /dev/sdaa, /dev/sdab...}
    QString preferredDev;   //the same as device
    bool isdrive;
    bool isusb;
    bool readonly;
    bool isboot;
    bool isram;
    bool isloop;
    bool israid;
    qulonglong size;
    QString drive;          //drive device name. such as sda, sdb, sdc...
    QString drivePath;      //such as /org/freedesktop/UDisks2/drives/ST1000DM003_1CH162_S1DB82SP
    QString driveId;        //driver id. such as ST1000DM003-1CH162-S1DB82SP
    QString uuid;   //only partition have uuid

    QStringList interface;
    QStringList symlinks;

    //filesystem property
    QStringList mountpoints;

    //partition property
    uint partnum;
    qulonglong partsize;
    qulonglong partoffset;
    QString parttype;
    QString partname;
    QString partuuid;
    QString parttable;
    //partitoin table property
    QString parttabtype;

    //raid info
    QString mdraid;
    QString mdraidMember;
};

class managedobject
{
public:
    managedobject();
    ~managedobject();

public:
    QString path;
    QMap<QString, drivedevice*> drivedev;
    QMap<QString, blockdevice*> blockdev;
    QMap<QString, mdraiddevice*> mdraiddev;
};

#include <QMetaType>
Q_DECLARE_METATYPE(InterfaceList)
Q_DECLARE_METATYPE(QList<QByteArray>)

#endif // DEVICEDEF_H
