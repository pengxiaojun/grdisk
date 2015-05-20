#ifndef COMMANDTOOL_H
#define COMMANDTOOL_H
#include <QObject>
#include "devicedef.h"

class commandtool
{
public:
    commandtool();
    //partiton related
    int create_part(const blockdevice *block, qulonglong start=0, qulonglong end=0);
    int remove_part(const blockdevice *block);
    int format_part(const blockdevice *block);
    int mount_part(const blockdevice *block, QString mount_point, QString uid, QString gid="");
    int umount_part(const blockdevice *block);
    int deautomnt_part(const blockdevice *block);
    int automnt_part(const blockdevice *block, QString mount_point);
    int df(QMap<QString, dfentry>& entries);
    int dport(QMap<QString, int>& ports);
    static int exec(QString command, QStringList args, QString& result, int timeout=30000);
private:
    static bool lookup_command(QString command, QString &path);

private:

};

#endif // COMMANDTOOL_H
