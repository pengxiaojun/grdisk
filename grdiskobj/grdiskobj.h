#ifndef GRDISKOBJ_H
#define GRDISKOBJ_H

#include <QObject>
#include <QStringList>
#include "iscsiobj.h"

//just for Udisk , no for Udisk2
class GrdiskObj : public QObject
{
    Q_OBJECT
public:
    explicit GrdiskObj( QObject *parent = 0);

public slots:
    int ProcessArgs(const QStringList& Aarglst);
    //void ShowHelp();
    void showHelp();
    int doAutoMount(QStringList args);
    int doDeautoMount(QStringList args);
    int ensureUmount(QString dev);
    int doMount(QStringList Arglst);
    int doFormat(QString dev);
    int doCreatePart(QString dev, QString partName, qulonglong start, qulonglong end);
    //void doDelPart(QString dev);
private:
    void readOpt();
    void log(QString str);
    bool preMount(QString dev, QString mnt);
private:
    bool listusb_;
    iscsiobj iscsi_;
};

#endif // GRDISKOBJ_H
