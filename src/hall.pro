#-------------------------------------------------
#
# Project created by QtCreator 2011-07-07T08:10:15
#
#-------------------------------------------------

QT       += core gui

TARGET = hall
TEMPLATE = app

LIBS += -L/usr/local/lib -lmsdp2xxx

INCLUDEPATH += /usr/local/include

SOURCES += main.cpp\
        mainwindow.cpp \
    configui.cpp \
    powerswitch.cpp

HEADERS  += mainwindow.h \
    configui.h \
    powerswitch.h

FORMS    += mainwindow.ui \
    configui.ui
