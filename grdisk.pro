#-------------------------------------------------
#
# Project created by QtCreator 2012-11-14T15:15:05
#
#-------------------------------------------------

QT       += core dbus

QT       -= gui

TARGET = grdisk
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    grdiskobj/grdiskobj.cpp \
    iscsiobj.cpp \
    diskmanager.cpp \
    devicedef.cpp \
    extractor.cpp \
    diskoperator.cpp \
    commandtool.cpp \
    util.cpp

HEADERS += \
    grdiskobj/grdiskobj.h \
    iscsiobj.h \
    diskmanager.h \
    devicedef.h \
    extractor.h \
    diskoperator.h \
    commandtool.h \
    util.h
