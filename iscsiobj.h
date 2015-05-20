#ifndef ISCSIOBJ_H
#define ISCSIOBJ_H

#include <QObject>


class iscsiobj : public QObject
{
    Q_OBJECT
public:
    explicit iscsiobj(QObject *parent = 0);
    int process(const QStringList& args);
private slots:
    void help();
    int discover(const QStringList& args);
    int login(const QStringList& args);
    int logout(const QStringList& args);
    int autologin(const QStringList& args);
    int setinitiatorname(QString name);
    int getinitiatorname();
};

#endif // ISCSIOBJ_H
