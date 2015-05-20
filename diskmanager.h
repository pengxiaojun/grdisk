#ifndef DISKMANAGER_H
#define DISKMANAGER_H

#include <QObject>
#include <QStringList>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusReply>
#include <QList>
#include <QByteArray>
#include "devicedef.h"
#include "extractor.h"
#include "diskoperator.h"
#include "iscsiobj.h"

class diskmanager: public QObject
{
    Q_OBJECT
public:
    diskmanager(QObject *parent=0);
    ~diskmanager();
    int process(const QStringList& args);
private:
    int initialize();
    void help(const QString& append="");
private:
    managedobject maobjects;
    extractor extrator;
    diskoperator op;
    iscsiobj iscsi;
};


#endif // DISKMANAGER_H
