#ifndef UTIL_H
#define UTIL_H

#include <QObject>
#include "devicedef.h"

extern grdisk_opt_t g_option;

class util
{
public:
    util();
    static bool lookup_command(QString command, QString &path);
    static void log(const QString& str);
    static void load_opt();
};

#endif // UTIL_H
