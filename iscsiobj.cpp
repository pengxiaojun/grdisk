#include "iscsiobj.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QtCore/QCoreApplication>
#include "util.h"
#include "commandtool.h"

#define SWAP_FILE           "iscsi.swap"
#define IRET_ALREADY_LOGIN  254
#define INITIATORNAME_FILE  "/etc/iscsi/initiatorname.iscsi"
#define INITIATORNAME_PREFIX "InitiatorName="
#define ISCSIADM_COMMAND    "iscsiadm"
#define SERVICE_COMMAND     "service"

iscsiobj::iscsiobj(QObject *parent) :
    QObject(parent)
{
}

int iscsiobj::process(const QStringList& args)
{
    if (args.size() < 3)
    {
        help();
        return GE_OK;
    }
    QString oper = args.at(2);
    if (oper == "discover")
    {
        return discover(args);
    }
    else if (oper == "login")
    {
        return login(args);
    }
    else if (oper == "autologin")
    {
        return autologin(args);
    }
    else if (oper == "logout")
    {
        return logout(args);
    }
    else if (oper == "set")
    {
        if (args.size() < 5)
        {
            help();
            return GE_OK;
        }
        QString arg = args.at(3);
        if (arg == "initiator")
        {
            return setinitiatorname(args.at(4));
        }
    }
    else if (oper == "get")
    {
        if (args.size() < 4)
        {
            help();
            return GE_OK;
        }
        QString arg = args.at(3);
        if (arg == "initiator")
        {
            return getinitiatorname();
        }
    }
    help();
    return GE_OK;
}

void iscsiobj::help()
{
    QString str("grdisk iscsi | discover | login | autologin | logout\n"
                "grdisk iscsi discover IP                    - discover target\n"
                "grdisk iscsi login IQN IP [USER] [PASS]     - login to target\n"
                "grdisk iscsi autologin IQN IP [USER] [PASS] - login to target\n"
                "grdisk iscsi logout IQN                     - logout from iqn\n"
                "grdisk iscsi set initiator INITIATORNAME    - set initiator name\n"
                "grdisk iscsi get initiator                  - get initiator name\n");
    qDebug()<<qPrintable(str);
}

int iscsiobj::discover(const QStringList &args)
{
    if (args.size() < 4)
    {
        help();
        return -1;
    }
    QStringList sl;
    sl << "-m" << "discovery" << "-t" << "sendtargets" << "-p" << args.at(3);
    QString result;
    int ret = commandtool::exec(ISCSIADM_COMMAND, sl, result);
    if (ret != 0)
    {
        util::log(QString("[iscsi]discover %1 error=%2 code=%3").arg(args.at(3)).arg(result).arg(ret));
        return -1;
    }
    //write
    QFile file(SWAP_FILE);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    if (!file.isOpen() || !file.isWritable())
        return -1;
    file.write(result.toUtf8());
    file.close();
    return 0;
}

int iscsiobj::login(const QStringList &args)
{
    if (args.size() < 5)
    {
        help();
        return -1;
    }

    QStringList sl;
    QString iqn = args.at(3);
    QString ip = args.at(4);
    QString user = "";
    QString pass = "";
    QString result;
    int exitCode;
    if (args.size() > 6)
    {
        user = args.at(5);
        pass = args.at(6);
    }
    bool chap = false;
    if (!user.isEmpty() && !pass.isEmpty())
    {
        chap = true;
        //CHAP login
        //step 1: modify login strategy
        sl << "-m" << "node" << "-T" << iqn << "-p" << ip << "-o" << "update" << "-n" << "node.session.auth.authmethod" << "-v" << "CHAP";
        exitCode = commandtool::exec(ISCSIADM_COMMAND, sl, result);
        if (exitCode != 0)
        {
            util::log(QString("[iscsi]chap login %1 user=%2 pass=%3 error=%4 code=%5").arg(ip).arg(user).arg(pass).arg(result).arg(exitCode));
            return -1;
        }
        //step 2: modify login user
        sl.clear();
        sl << "-m" << "node" << "-T" << iqn << "-p" << ip << "-o" << "update" << "-n" << "node.session.auth.username" << "-v" << user;
        exitCode = commandtool::exec(ISCSIADM_COMMAND, sl, result);
        if (exitCode != 0)
        {
            util::log(QString("[iscsi]chap set user %1 user=%2 pass=%3 error=%4 code=%5").arg(ip).arg(user).arg(pass).arg(result).arg(exitCode));
            return -1;
        }

        //step 3: modify login pass
        sl.clear();
        sl << "-m" << "node" << "-T" << iqn << "-p" << ip << "-o" << "update" << "-n" << "node.session.auth.password" << "-v" << pass;
        exitCode = commandtool::exec(ISCSIADM_COMMAND, sl, result);
        if (exitCode != 0)
        {
            util::log(QString("[iscsi]chap set password %1 user=%2 pass=%3 error=%4 code=%5").arg(ip).arg(user).arg(pass).arg(result).arg(exitCode));
            return -1;
        }
    }

    sl.clear();
    sl << "-m" << "node" << "-T" << iqn << "-p" << ip << "-l";
    bool logouted = false;
again:
    exitCode = commandtool::exec(ISCSIADM_COMMAND, sl, result);
    if (exitCode != 0 && exitCode != IRET_ALREADY_LOGIN)
    {
        util::log(QString("[iscsi]login(%1) %2 error=%3 code=%4").arg(chap ? "chap" : "anonymous").arg(ip).arg(result).arg(exitCode));
        //try to logout and continue to login
        if (!logouted)
        {
            util::log(QString("[iscsi]login(%1) try to logout %2").arg(chap ? "chap" : "anonymous").arg(ip));
            logout(args);
            logouted = true;
            goto again;
        }
        return -1;
    }
    //login success
    util::log(QString("[iscsi]login %1 %2 success").arg(ip).arg(iqn));
    return 0;
}

int iscsiobj::logout(const QStringList &args)
{
    if (args.size() < 4)
    {
        help();
        return -1;
    }

    QStringList sl;
    QString iqn = args.at(3);
    QString result;
    int exitCode;

    sl << "-m" << "node" << "-T" << iqn << "-u";
    exitCode = commandtool::exec(ISCSIADM_COMMAND, sl, result);
    if (exitCode != 0)
    {
        util::log(QString("[iscsi]logout %1 error=%2 code=%3").arg(iqn).arg(result).arg(exitCode));
        return -1;
    }

    util::log(QString("[iscsi]logout %1 success").arg(iqn));
    return 0;
}

int iscsiobj::autologin(const QStringList &args)
{
    int ret = login(args);
    if (ret != 0)
        return ret;


    QStringList sl;
    QString iqn = args.at(3);
    QString ip = args.at(4);
    QString result;
    int exitCode;

    sl << "-m" << "node" << "-T" << iqn << "-p" << ip << "-o" << "update" << "-n" << "node.startup" << "-v" << "automatic";
    exitCode = commandtool::exec(ISCSIADM_COMMAND, sl, result);
    if (exitCode != 0)
    {
        util::log(QString("[iscsi]autologin %1 error=%2 code=%3").arg(iqn).arg(result).arg(exitCode));
        return -1;
    }
    return 0;
}

int iscsiobj::setinitiatorname(QString name)
{
    //InitiatorName=iqn.1996-04.de.suse:01:e6ca28cc9f20

    QString prefix(INITIATORNAME_PREFIX);

    QFile file(INITIATORNAME_FILE);
    if (!file.open(QFile::ReadOnly))
    {
        util::log(QString("[iscsi]open %1 for read error").arg(INITIATORNAME_FILE));
        return GE_ERROR;
    }
    QTextStream its(&file);
    QString line;
    QString content;

    do{
        line = its.readLine();
        if (line.startsWith(prefix))
        {
            //comment old initiatorname
            line = line.replace(prefix, "#"+prefix);
            content += (line +"\n");
        }
        else{
            if (line.indexOf(prefix) == -1 && line.length() > 0)
                content += (line + "\n");
        }
    }while (!line.isNull());
    file.close();

    content += QString("%1%2\n").arg(INITIATORNAME_PREFIX).arg(name);

    if (!file.open(QFile::WriteOnly | QFile::Truncate))
    {
        util::log(QString("[iscsi]open %1 for write error").arg(INITIATORNAME_FILE));
        return GE_ERROR;
    }
    QTextStream ots(&file);
    ots << content;
    file.close();

    //restart iscsi service
    int exitCode;
    QString result;
    QStringList args;
    QString iscsi_service_name = "iscsid";
    QString open_iscsi_serice_name = "open-iscsi";
    QString service_name = iscsi_service_name;
 again:
    args.clear();
    args << service_name << "restart";
    exitCode = commandtool::exec(SERVICE_COMMAND, args, result);
    if (exitCode != 0 && service_name != open_iscsi_serice_name)
    {
        service_name = open_iscsi_serice_name;
        goto again;
    }
    util::log(QString("[iscsi]set initiator name %1 exit code %2").arg(name).arg(exitCode));
    return exitCode;
}

int iscsiobj::getinitiatorname()
{
    QString prefix(INITIATORNAME_PREFIX);
    QString line;
    QFile file(INITIATORNAME_FILE);
    if (file.open(QFile::ReadOnly))
    {
        QTextStream ts(&file);

        do{
            line = ts.readLine();
            if (line.startsWith(prefix))
            {
                line = line.remove(0, prefix.length());
                break;
            }
        }while (!line.isNull());
    }
    else{
        util::log(QString("[iscsi]open %1 for read error").arg(INITIATORNAME_FILE));
    }

    qDebug()<< line;

    QFile data(SWAP_FILE);
    if (data.open(QFile::WriteOnly | QFile::Truncate))
    {
        QTextStream out(&data);
        out << line;
        data.close();
    }
    return 0;
}
