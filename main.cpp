#include <QtCore/QCoreApplication>
#include "grdiskobj/grdiskobj.h"
#include "diskmanager.h"
#include "util.h"
#include <QStringList>
#include <stdio.h>
#include <QDebug>
#include <stdlib.h>

 void myMessageOutput(QtMsgType, const char *msg)
 {
     fprintf(stdout,"%s\n",msg);
 }

int main(int argc, char *argv[])
{
    qInstallMsgHandler(myMessageOutput);

    QCoreApplication a(argc, argv);
    util::load_opt();

    diskmanager *dm = new diskmanager();
    GrdiskObj* diskObj = new GrdiskObj();  // just for Udisk

    if (g_option.use_udisk)
    {
        return diskObj->ProcessArgs(a.arguments());
    }

    //try udisk2 first
    int ret = dm->process(a.arguments());
    if (ret == ERR_NO_UDISK2)
    {
        ret = diskObj->ProcessArgs(a.arguments());
    }
    return ret;
}
